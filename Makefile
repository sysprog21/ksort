MODULENAME := ksort
obj-m += $(MODULENAME).o
$(MODULENAME)-y += main.o sort.o

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

insmod: all
	sudo insmod $(MODULENAME).ko

rmmod:
	sudo rmmod $(MODULENAME)

check:
	$(MAKE) insmod
	sudo ./user
	$(MAKE) rmmod

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) user
