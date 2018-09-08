#include "extensions/filters/http/transformation/transformation_filter.h"

#include "common/common/empty_string.h"
#include "common/common/enum_to_int.h"
#include "common/config/metadata.h"
#include "common/http/solo_filter_utility.h"
#include "common/http/utility.h"

#include "extensions/filters/http/transformation/body_header_transformer.h"
#include "extensions/filters/http/transformation/transformer.h"
#include "extensions/filters/http/transformation_well_known_names.h"

namespace Envoy {
namespace Http {

TransformationFilter::TransformationFilter() {}

TransformationFilter::~TransformationFilter() {}

void TransformationFilter::onDestroy() { resetInternalState(); }

FilterHeadersStatus TransformationFilter::decodeHeaders(HeaderMap &header_map,
                                                        bool end_stream) {

  checkRequestActive();

  if (is_error()) {
    return FilterHeadersStatus::StopIteration;
  }

  if (!requestActive()) {
    return FilterHeadersStatus::Continue;
  }

  request_headers_ = &header_map;

  if (end_stream || isPassthrough(*request_transformation_)) {
    transformRequest();

    return is_error() ? FilterHeadersStatus::StopIteration
                      : FilterHeadersStatus::Continue;
  }

  return FilterHeadersStatus::StopIteration;
}

FilterDataStatus TransformationFilter::decodeData(Buffer::Instance &data,
                                                  bool end_stream) {
  if (!requestActive()) {
    return FilterDataStatus::Continue;
  }

  request_body_.move(data);
  if ((decoder_buffer_limit_ != 0) &&
      (request_body_.length() > decoder_buffer_limit_)) {
    error(Error::PayloadTooLarge);
    requestError();
    return FilterDataStatus::StopIterationNoBuffer;
  }

  if (end_stream) {
    transformRequest();
    return is_error() ? FilterDataStatus::StopIterationNoBuffer
                      : FilterDataStatus::Continue;
  }

  return FilterDataStatus::StopIterationNoBuffer;
}

FilterTrailersStatus TransformationFilter::decodeTrailers(HeaderMap &) {
  if (requestActive()) {
    transformRequest();
  }
  return is_error() ? FilterTrailersStatus::StopIteration
                    : FilterTrailersStatus::Continue;
}

FilterHeadersStatus TransformationFilter::encodeHeaders(HeaderMap &header_map,
                                                        bool end_stream) {

  checkResponseActive();

  if (!responseActive()) {
    // this also covers the is_error() case. as is_error() == true implies
    // responseActive() == false
    return FilterHeadersStatus::Continue;
  }

  response_headers_ = &header_map;

  if (end_stream || isPassthrough(*response_transformation_)) {
    transformResponse();
    return FilterHeadersStatus::Continue;
  }

  return FilterHeadersStatus::StopIteration;
}

FilterDataStatus TransformationFilter::encodeData(Buffer::Instance &data,
                                                  bool end_stream) {
  if (!responseActive()) {
    return FilterDataStatus::Continue;
  }

  response_body_.move(data);
  if ((encoder_buffer_limit_ != 0) &&
      (response_body_.length() > encoder_buffer_limit_)) {
    error(Error::PayloadTooLarge);
    responseError();
    return FilterDataStatus::Continue;
  }

  if (end_stream) {
    transformResponse();
    return FilterDataStatus::Continue;
  }

  return FilterDataStatus::StopIterationNoBuffer;
}

FilterTrailersStatus TransformationFilter::encodeTrailers(HeaderMap &) {
  if (responseActive()) {
    transformResponse();
  }
  return FilterTrailersStatus::Continue;
}

const std::string &
TransformationFilter::directionToKey(TransformationFilter::Direction d) {
  switch (d) {
  case TransformationFilter::Direction::Request:
    return Config::MetadataTransformationKeys::get().REQUEST_TRANSFORMATION;
  case TransformationFilter::Direction::Response:
    return Config::MetadataTransformationKeys::get().RESPONSE_TRANSFORMATION;
  default:
    NOT_REACHED_GCOVR_EXCL_LINE;
  }
}

void TransformationFilter::checkRequestActive() {
  route_ = decoder_callbacks_->route();
  request_transformation_ =
      getTransformFromRoute(TransformationFilter::Direction::Request);
}

void TransformationFilter::checkResponseActive() {
  route_ = encoder_callbacks_->route();
  response_transformation_ =
      getTransformFromRoute(TransformationFilter::Direction::Response);
}

const envoy::api::v2::filter::http::Transformation *
TransformationFilter::getTransformFromRoute(
    TransformationFilter::Direction direction) {

  if (!route_) {
    return nullptr;
  }

  const auto *config = SoloFilterUtility::resolvePerFilterConfig<
      RouteTransformationFilterConfig>(
      Config::TransformationFilterNames::get().TRANSFORMATION, route_);

  if (config != nullptr) {
    switch (direction) {
    case TransformationFilter::Direction::Request:
      return config->getRequestTranformation();
    case TransformationFilter::Direction::Response:
      return config->getResponseTranformation();
    default:
      // TODO(yuval-k): should this be a warning log?
      NOT_REACHED_GCOVR_EXCL_LINE;
    }
  }
  return nullptr;
}

void TransformationFilter::transformRequest() {
  transformSomething(&request_transformation_, *request_headers_, request_body_,
                     &TransformationFilter::requestError,
                     &TransformationFilter::addDecoderData);
}

void TransformationFilter::transformResponse() {
  transformSomething(&response_transformation_, *response_headers_,
                     response_body_, &TransformationFilter::responseError,
                     &TransformationFilter::addEncoderData);
}

void TransformationFilter::addDecoderData(Buffer::Instance &data) {
  decoder_callbacks_->addDecodedData(data, false);
}

void TransformationFilter::addEncoderData(Buffer::Instance &data) {
  encoder_callbacks_->addEncodedData(data, false);
}

void TransformationFilter::transformSomething(
    const envoy::api::v2::filter::http::Transformation **transformation,
    HeaderMap &header_map, Buffer::Instance &body,
    void (TransformationFilter::*responeWithError)(),
    void (TransformationFilter::*addData)(Buffer::Instance &)) {

  switch ((*transformation)->transformation_type_case()) {
  case envoy::api::v2::filter::http::Transformation::kTransformationTemplate:
    transformTemplate((*transformation)->transformation_template(), header_map,
                      body, addData);
    break;
  case envoy::api::v2::filter::http::Transformation::kHeaderBodyTransform:
    transformBodyHeaderTransformer(header_map, body, addData);
    break;
  case envoy::api::v2::filter::http::Transformation::
      TRANSFORMATION_TYPE_NOT_SET:
    // no transformation is set, just re-add the body. this shouldnt happen.
    (this->*addData)(body);
    break;
  }

  *transformation = nullptr;
  if (is_error()) {
    (this->*responeWithError)();
  }
}

void TransformationFilter::transformTemplate(
    const envoy::api::v2::filter::http::TransformationTemplate &transformation,
    HeaderMap &header_map, Buffer::Instance &body,
    void (TransformationFilter::*addData)(Buffer::Instance &)) {
  try {
    Transformer transformer(transformation);
    transformer.transform(header_map, body);

    if (body.length() > 0) {
      (this->*addData)(body);
    } else {
      header_map.removeContentType();
    }
  } catch (nlohmann::json::parse_error &e) {
    // json may throw parse error
    error(Error::JsonParseError, e.what());
  } catch (std::runtime_error &e) {
    // inja may throw runtime error
    error(Error::TemplateParseError, e.what());
  }
}

void TransformationFilter::transformBodyHeaderTransformer(
    HeaderMap &header_map, Buffer::Instance &body,
    void (TransformationFilter::*addData)(Buffer::Instance &)) {
  try {
    BodyHeaderTransformer transformer;
    transformer.transform(header_map, body);

    if (body.length() > 0) {
      (this->*addData)(body);
    } else {
      header_map.removeContentType();
    }
  } catch (nlohmann::json::parse_error &e) {
    // json may throw parse error
    error(Error::JsonParseError, e.what());
  }
}

void TransformationFilter::requestError() {
  ASSERT(is_error());
  decoder_callbacks_->sendLocalReply(error_code_, error_messgae_, nullptr);
}

void TransformationFilter::responseError() {
  ASSERT(is_error());
  response_headers_->Status()->value(enumToInt(error_code_));
  Buffer::OwnedImpl data(error_messgae_);
  response_headers_->removeContentType();
  response_headers_->insertContentLength().value(data.length());
  encoder_callbacks_->addEncodedData(data, false);
}

void TransformationFilter::resetInternalState() {
  request_body_.drain(request_body_.length());
  response_body_.drain(response_body_.length());
}

void TransformationFilter::error(Error error, std::string msg) {
  error_ = error;
  resetInternalState();
  switch (error) {
  case Error::PayloadTooLarge: {
    error_messgae_ = "payload too large";
    error_code_ = Code::PayloadTooLarge;
    break;
  }
  case Error::JsonParseError: {
    error_messgae_ = "bad request";
    error_code_ = Code::BadRequest;
    break;
  }
  case Error::TemplateParseError: {
    error_messgae_ = "bad request";
    error_code_ = Code::BadRequest;
    break;
  }
  case Error::TransformationNotFound: {
    error_messgae_ = "transformation for function not found";
    error_code_ = Code::NotFound;
    break;
  }
  }
  if (!msg.empty()) {
    if (error_messgae_.empty()) {
      error_messgae_ = std::move(msg);
    } else {
      error_messgae_ = error_messgae_ + ": " + msg;
    }
  }
}

bool TransformationFilter::is_error() { return error_.has_value(); }

} // namespace Http
} // namespace Envoy
