export KDIR

all:
	mkdir -p build
	$(MAKE) -C ./hid-tmx all
	cp ./hid-tmx/hid-tmx.ko ./build
	$(MAKE) -C ./hid-tminit all
	cp ./hid-tminit/hid-tminit.ko ./build
	
clean:
	$(MAKE) -C ./hid-tmx clean
	$(MAKE) -C ./hid-tminit clean
	rm -r ./build/*
