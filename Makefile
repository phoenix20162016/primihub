
TARGET := //:node \
         //:cli \
         //src/primihub/cli:reg_cli \
         //src/primihub/pybind_warpper:linkcontext \
         //src/primihub/pybind_warpper:opt_paillier_c2py \
         //:py_main

JOBS?=
ifneq ($(jobs), )
	JOBS = $(jobs)
	BUILD_FLAG += --jobs=$(JOBS)
endif

ifeq ($(tee), y)
	BUILD_FLAG += --cxxopt=-DSGX
endif

ifeq ($(debug), y)
	BUILD_FLAG += --config=linux_asan
endif

release:
	bazel build --config=PLATFORM_HARDWARE $(BUILD_FLAG) ${TARGET}

#linux_x86_64:
#	bazel build --config=linux_x86_64 $(BUILD_FLAG) ${TARGET}
#
#linux_aarch64:
#	bazel build --config=linux_aarch64 ${TARGET}
#
#macos_arm64:
#	bazel build --config=darwin_arm64 ${TARGET}
#
#macos_x86_64:
#	bazel build --config=darwin_x86_64 ${TARGET}

.PHONY: clean

clean:
	bazel clean
