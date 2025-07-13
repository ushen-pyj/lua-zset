all : lua-zset

linux : zset.so

clean :
	rm -f zset.so lua-zset.dll

zset.so : lua-zset.c zset.c
	gcc -Wall -g -o $@ -fPIC --shared $^ -I. -I/usr/include/lua5.4

zset : zset.c
	gcc -Wall -g -o $@ $^ -I.

lua-zset : lua-zset.c zset.c
	gcc -Wall -g -o $@.dll -fPIC --shared $^ -I. -I/usr/include/lua5.4 -L/usr/local/bin -llua5.4
