all: dev

TARGET := $(shell rustup toolchain list|grep default|grep -o '[^ ]*linux[^ ]*'|sed 's/-unknown//'|sed 's/^[a-z]\+-//')
RUST_TARGET := $(shell echo $(TARGET)|sed 's/-linux/-unknown-linux/')

PROJECT_PATH := $(shell pwd)
TARGET_ROOT := $(HOME)/x-tools/$(TARGET)
SYSROOT := $(TARGET_ROOT)/$(TARGET)/sysroot

INSTALL_PATH := $(PROJECT_PATH)/install_dir/$(TARGET)

ROCKER_SERVER_UAU_ADDR = $(shell head -1 /dev/urandom | hexdump | head -1 | sed 's/[0 ]//g')

dev: install

release: install_release

base:
ifdef UAU_REFRESH
	printf "$(ROCKER_SERVER_UAU_ADDR)" > rocker_server/src/rocker_server_uau_addr_cfg;
	printf "#define ROCKER_SERVER_UAU_ADDR \"`cat rocker_server/src/rocker_server_uau_addr_cfg`\"" \
		> librocker_client/src/rocker_server_uau_addr_cfg.h
endif
	echo "\"$(INSTALL_PATH)/librocker_client/lib\"" > librocker_client_wrapper/ld_path
	-@ ln -sv $(TARGET_ROOT)/bin/* $(HOME)/.cargo/bin/ 2>/dev/null
	mkdir -p librocker_client/build
	-@ rm -rf $(INSTALL_PATH)
	mkdir -p $(INSTALL_PATH)

compile: base
	cd librocker_client/build; \
		cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=../cmake_toolchain/$(TARGET).cmake; \
		make; \
		make install
	mv librocker_client/install_dir $(INSTALL_PATH)/librocker_client
	cargo build --target=$(RUST_TARGET)

install_base:
	cp tests/test_on_target.sh $(INSTALL_PATH)/

install: compile install_base
	cp target/$(RUST_TARGET)/debug/rocker_server $(INSTALL_PATH)/

compile_release: base
	cd librocker_client/build; \
		cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../cmake_toolchain/$(TARGET).cmake; \
		make; \
		make install
	cargo build --target=$(RUST_TARGET) --release

install_release: compile_release install_base
	cp target/$(RUST_TARGET)/release/rocker_server $(INSTALL_PATH)/

test: install
	cargo install -f --bins --path=rocker_server
	-@ kill -9 `ps axo pid,args | grep rocker_server | grep -v grep | grep -o '[0-9]\+'`
	cargo test --target=$(RUST_TARGET) -- --nocapture --test-threads=1
ifeq (0, $(shell id -u))
	cargo test --target=$(RUST_TARGET) -- --nocapture --ignored --test-threads=1
endif
	-@ kill -9 `ps axo pid,args | grep rocker_server | grep -v grep | grep -o '[0-9]\+'`
	$(MAKE) test -w -C librocker_client/build

bench: install_release
	# TODO

clean:
	-@ cargo clean
	-@ rm -rf install_dir
	-@ rm -rf librocker_client/{install_dir,build}
