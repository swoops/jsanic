# jsanic

If you want your production JavaScript looking sharp, then use js-beautify. Are
you more concerned with going fast than looking pretty? Then jsanic is the tool
for you.

Check out this overly contrived example:

```sh
> for i in {1..100000}; do echo "a = {"; done | /usr/bin/time -f 'seconds: %e mem: %M kb' js-beautify
Command terminated by signal 9
seconds: 39.28 mem: 13323876 kb
for i in {1..100000}; do echo "a = {"; done | /usr/bin/time -f 'seconds: %e mem: %M kb' ./jsanic >/dev/null
seconds: 0.50 mem: 32080 kb
```

# why

A while back I was testing a website that had a single 5.6MB JS file. The file
was minified. At the time I had trouble finding a tool that could beautify it
without being killed by the kernel. Even Firefox would crash in attempting to
beautify it. Learning to parse JS sounded fun anyways, so I wrote this tool.

The file in question ended up getting beautified by js-beautify. My tool will
run faster and with lower mem usage though.

```sh
> /usr/bin/time -f 'seconds: %e mem: %M kb' js-beautify /tmp/js_file.js > out.js
seconds: 28.40 mem: 470208 kb
> /usr/bin/time -f 'seconds: %e mem: %M kb' ./jsanic /tmp/js_file.js > out.js
seconds: 1.85 mem: 68544 kb
```

# goal

Once something is used, it is freed. The scanner (`cache.c`) will only read
from the file once, and won't store any more of the file in memory than is
necessary. The lexer (`tokenizer.c`) will build a thread safe double linked
list of tokens. A separate thread can then pop from the tokens as they become
available.

Eventually I want to create something of an AST so that I can replace minified
variables with unique, memorable nouns according to scope.
