# ksort

A Linux kernel module that creates the device `/dev/sort`, capable of performing concurrent sorts.

## User access to the character device

In order to access the character device as a user you need to add a rule to
udev with the included rules file.
```
$ sudo cp 99-xoro.rules /etc/udev/rules.d/
$ sudo udevadm control --reload
```

## Build and Test

Build from source:
```shell
$ make
```

Test:
```shell
$ make check
```

You should see also more messages in the kernel log.

## References
* [The Linux Kernel Module Programming Guide](https://sysprog21.github.io/lkmpg/)
* [Writing a simple device driver](https://www.apriorit.com/dev-blog/195-simple-driver-for-linux-os)
* [Character device drivers](https://linux-kernel-labs.github.io/refs/heads/master/labs/device_drivers.html)
* [cdev interface](https://lwn.net/Articles/195805/)
* [Character device files](https://sysplay.in/blog/linux-device-drivers/2013/06/character-device-files-creation-operations/)
* [Linux Workqueue](https://www.kernel.org/doc/html/latest/core-api/workqueue.html)

## License

`ksort`is released under the MIT license. Use of this source code is governed by
a MIT-style license that can be found in the `LICENSE` file.
