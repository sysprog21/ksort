obj-m += ksort.o
ksort-objs := \
    sort_mod.o \
    sort.o

obj-m += xoro.o
xoro-objs := \
    xoro_mod.o

GIT_HOOKS := .git/hooks/applied

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: $(GIT_HOOKS) user
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

user: user.c
	$(CC) -o $@ $^

test_xoro: test_xoro.c
	$(CC) -o $@ $^

insmod: all rmmod
	sudo insmod ksort.ko
	sudo insmod xoro.ko

rmmod:
	@sudo rmmod ksort 2>/dev/null || echo
	@sudo rmmod xoro 2>/dev/null || echo

check: user test_xoro
	$(MAKE) insmod
	sudo ./user
	sudo ./test_xoro
	$(MAKE) rmmod

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) user test_xoro
