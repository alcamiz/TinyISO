test-iter:
	@mkdir -p bin
	@gcc -g -I include test/iter.c src/tni.c -o bin/iso_iter -liconv
