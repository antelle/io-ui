all: download-deps build-app build-examples

download-deps:
	node download-deps.js

build-app:
	node build.js

build-examples:
	node build-examples.js

run-tests:
	node node_modules/nodeunit/bin/nodeunit

clean:
	rm -rf build