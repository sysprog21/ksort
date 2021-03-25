CONFIG_MODULE_SIG = n
TARGET_MODULE := ksort

obj-m := $(TARGET_MODULE).o
ksort-objs := \
	xoroshiro128plus.o \
	heap.o \
	main.o

ccflags-y := -std=gnu99 -Wno-declaration-after-statement

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

GIT_HOOKS := .git/hooks/applied

all: $(GIT_HOOKS) test_xoro
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

test_xoro: test_xoro.c
	$(CC) -o $@ $^

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) test_xoro

load:
	sudo insmod $(TARGET_MODULE).ko
unload:
	sudo rmmod $(TARGET_MODULE) || true >/dev/null

PRINTF = env printf
PASS_COLOR = \e[32;01m
NO_COLOR = \e[0m
pass = $(PRINTF) "$(PASS_COLOR)$1 Passed [-]$(NO_COLOR)\n"

check: all
	$(MAKE) unload
	sudo dmesg -C
	$(MAKE) load
	@sudo ./test_xoro && $(call pass)
	@dmesg | grep "test passed" >/dev/null && $(call pass)
	$(MAKE) unload
