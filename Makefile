bazel-bin/envoy:
	bazel build -c opt :envoy --jobs=$$[$(shell nproc --all)-2]

bazel-bin/envoy.debuginfo: bazel-bin/envoy
	objcopy --only-keep-debug bazel-bin/envoy bazel-bin/envoy.debuginfo

bazel-bin/envoy.stripped: bazel-bin/envoy
	strip -g bazel-bin/envoy -o bazel-bin/envoy.stripped

.PHONY: upload-debug
upload-debug: bazel-bin/envoy.debuginfo ./bazel-bin/envoy
	aws s3 cp bazel-bin/envoy.debuginfo s3://artifacts.solo.io/$(shell readelf -n ./bazel-bin/envoy|grep "Build ID:" |cut -f2 -d:|tr -d ' ')/envoy.debuginfo

.PHONY: build-docker
build-docker: bazel-bin/envoy.stripped
	cp bazel-bin/envoy.stripped ci/
	cd ci && docker build -t soloio/envoy-gloo:$(shell readelf -n bazel-bin/envoy.stripped|grep "Build ID:" |cut -f2 -d:|tr -d ' ') .