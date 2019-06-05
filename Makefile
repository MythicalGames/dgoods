.PHONY: clean
clean:
	rm -rf build

.PHONY: build
build:
	mkdir -p build
	cd build && cmake ../ && make
	mv build/dgoods/dgoods* .
	rm -rf build/*
	mkdir -p build/dgoods
	mv dgoods.abi dgoods.wasm build/dgoods/
