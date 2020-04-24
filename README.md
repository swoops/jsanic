# jsanic

If you want your production JavaScript looking sharp, then use js-beautify. Are
you more concerned with going fast than looking pretty? Then jsanic is the tool
for you.

Check out this overly contrived example:

```sh
> for i in {1..100000}; do echo "a = {"; done | /usr/bin/time -f 'seconds: %e mem: %M kb' ./jsanic -i >/dev/null
seconds: 0.91 mem: 3164 kb
> for i in {1..100000}; do echo "a = {"; done | /usr/bin/time -f 'seconds: %e mem: %M kb' js-beautify >/dev/null
Command terminated by signal 9
seconds: 13.70 mem: 2986796 kb
```

# why

While testing a website, I ran into a 5.6 MB JavaScript file. My system is not
weak, it has an Intel i7 and 16G of ram and I run a minimal desktop. Yet every
beautifier I threw at this file crashed after prolonged execution. Eventually
js-beautify (a great tool) got it after I closed all other resource hogs
(Firefox, Java, Chrome).  It took a long time and there were still many lines
that were thousands of characters long.

I want something better and I want to learn about compilers anyway. May as well
write my own parser. 

# goal

Once something is used, it is freed. The scanner (`cache.c`) will only read
from the file once, and won't store any more of the file in memory than is
necessary. The lexer (`tokenizer.c`) will build a thread safe double linked
list of tokens. A separate thread can then pop from the tokens as they become
available.

Eventually I want to create something of an AST so that I can replace minified
variables with unique, memorable nouns according to scope.
