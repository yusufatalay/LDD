This pseudo device driver only supports single device instance.
It implements read, write, seek functions.

##Usage (Kernel Version < 6.4)
```
  make clean
  make (host | all)
  insmod pcd.ko
```
If you have older kernel version ( <= 6.3), then remove THIS_MODULE parameter from class_create.



