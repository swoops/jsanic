# jsanic

Fast (incomplete) JavaScript lexer. Eventually a beautifyer (hopefully).

```sh
> /usr/bin/time -f 'seconds: %e mem: %M kb' ps aux > /dev/null
seconds: 0.01 mem: 3428 kb
> /usr/bin/time -f 'seconds: %e mem: %M kb' ./jsanic -s /tmp/js_file.js
count:          2752225
lines:            96680
characters:     5603600
loops:            32773
variables:            0
ifs:               1103
ternary:           6302
unkown tokens:        0
seconds: 4.23 mem: 1920 kb
```

This will be fast, but probably not very pretty. I am not a developer. I am
mostly writing this for experience in more complicated programming problems.
Hopefully something useable comes out of it. Use at your own risk.

# why

While testing a website, I ran into a 5.6 MB JavaScript file. My system is not
weak, it has an Intel i7 and 16G of ram and I run a minimal desktop. Yet every
beautifier I threw at this file crashed after prolonged execution. Eventually
js-beautify (a great tool) got it after I closed all other resource hogs
(Firefox, Java, Chrome).  It took a long time and there were still many lines
that were thousands of characters long.

I want something better and I want to learn about compilers anyways. May as
well write my own parser. 

# goal
Once something is used, it is freed. The scanner (`cache.c`) will only read
from the file once, and won't store any more of the file in memory then is
necessary. The lexer (`tokenizer.c`) will build a thread safe double linked
list of tokens. A separate thread can then pop from the tokens as they become
available. Once a token is put in the token is freed. Once a branch is
beautified, it is freed.
