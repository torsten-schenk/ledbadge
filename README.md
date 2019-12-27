# ledbadge
Sertronics LED Tag Linux command line utility. Can upload monochrome .png-files to the device.

## Prerequisites

The following libraries are required in order to compile the program:

- libpng
- hidapi or hidapi-hidraw

## Compilation

Just execute make:

```
make
```

## Example usage

List devices, options and image requirements:

```
ledbadge -l
```

Upload an image:
```
ledbadge hello.png
```

Upload an image and set brightness 2, speed 7 and mode *laser*:
```
ledbadge hello.png -b 2 -s 7 -m laser
```
 
Upload an image and split it into two messages, the first being 2*char_width and the second being 1*char_width pixels in width:
```
ledbadge hello.png -e 2 -e 1
```

