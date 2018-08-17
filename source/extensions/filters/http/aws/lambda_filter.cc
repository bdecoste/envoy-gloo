#include "extensions/filters/http/aws/lambda_filter.h"
#include "extensions/filters/http/lambda_well_known_names.h"

#include <algorithm>
#include <list>
#include <string>
#include <vector>

#include "envoy/http/header_map.h"

#include "common/common/empty_string.h"
#include "common/common/hex.h"
#include "common/common/utility.h"
#include "common/http/filter_utility.h"
#include "common/http/headers.h"
#include "common/http/solo_filter_utility.h"
#include "common/http/utility.h"

namespace Envoy {
namespace Http {

const LowerCaseString LambdaFilter::INVOCATION_TYPE("x-amz-invocation-type");
const std::string LambdaFilter::INVOCATION_TYPE_EVENT("Event");
const std::string LambdaFilter::INVOCATION_TYPE_REQ_RESP("RequestResponse");
const LowerCaseString LambdaFilter::LOG_TYPE("x-amz-log-type");
const std::string LambdaFilter::LOG_NONE("None");

LambdaFilter::LambdaFilter(Upstream::ClusterManager& cluster_manager)
    : cluster_manager_(cluster_manager) {}

LambdaFilter::~LambdaFilter() {}

std::string LambdaFilter::functionUrlPath() {

  std::stringstream val;
  val << "/2015-03-31/functions/" << (function_on_route_->name())
      << "/invocations";
  const auto& qualifier = function_on_route_->qualifier();
  if (!qualifier.empty()) {
    val << "?Qualifier=" << qualifier;
  }
  return val.str();
}

FilterHeadersStatus LambdaFilter::decodeHeaders(HeaderMap &headers,
                                                bool end_stream) {

  protocol_options_ = SoloFilterUtility::resolveProtocolOptions<const LambdaProtocolExtensionConfig>(Config::LambdaHttpFilterNames::get().LAMBDA, decoder_callbacks_, cluster_manager_);
  if (! protocol_options_) {
    return FilterHeadersStatus::Continue;
  }

  route_ = decoder_callbacks_->route();
  // great! this is an aws cluster. get the function information:
  function_on_route_ = SoloFilterUtility::resolvePerFilterConfig<LambdaRouteConfig>(Config::LambdaHttpFilterNames::get().LAMBDA, route_);

  if (!function_on_route_) {
        decoder_callbacks_->sendLocalReply(Code::NotFound, "no function present for AWS upstream",
                                       nullptr);
    return FilterHeadersStatus::StopIteration;
  }

  aws_authenticator_.init(&protocol_options_->access_key(),
                          &protocol_options_->secret_key());
  request_headers_ = &headers;

  request_headers_->insertMethod().value().setReference(
      Headers::get().MethodValues.Post);

  //  request_headers_->removeContentLength();
  request_headers_->insertPath().value(functionUrlPath());

  if (end_stream) {
    lambdafy();
    return FilterHeadersStatus::Continue;
  }

  return FilterHeadersStatus::StopIteration;
}

FilterDataStatus LambdaFilter::decodeData(Buffer::Instance &data,
                                          bool end_stream) {
  if (! function_on_route_) {
    return FilterDataStatus::Continue;
  }

  aws_authenticator_.updatePayloadHash(data);

  if (end_stream) {
    lambdafy();
    return FilterDataStatus::Continue;
  }

  return FilterDataStatus::StopIterationAndBuffer;
}

FilterTrailersStatus LambdaFilter::decodeTrailers(HeaderMap &) {
  if (! function_on_route_) {
    return FilterTrailersStatus::Continue;
  }

  lambdafy();

  return FilterTrailersStatus::Continue;
}

void LambdaFilter::lambdafy() {
  static std::list<LowerCaseString> headers;

  const std::string &invocation_type = function_on_route_->async()
                                           ? INVOCATION_TYPE_EVENT
                                           : INVOCATION_TYPE_REQ_RESP;
  headers.push_back(INVOCATION_TYPE);
  request_headers_->addReference(INVOCATION_TYPE, invocation_type);

  headers.push_back(LOG_TYPE);
  request_headers_->addReference(LOG_TYPE, LOG_NONE);

  // TOOO(yuval-k) constify this and change the header list to
  // ref-or-inline like in header map
  headers.push_back(LowerCaseString("host"));
  request_headers_->insertHost().value(protocol_options_->host());

  headers.push_back(LowerCaseString("content-type"));

  aws_authenticator_.sign(request_headers_, std::move(headers),
                          protocol_options_->region());
  cleanup();
}

void LambdaFilter::cleanup() {
  request_headers_ = nullptr;
  function_on_route_ = nullptr;
  protocol_options_.reset();
}

} // namespace Http
} // namespace Envoy
