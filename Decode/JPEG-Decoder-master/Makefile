all: jpeg_decoder

jpeg_decoder: src/decoder.c
	gcc src/decoder.c -O2 -lm -o jpeg_decoder

test: jpeg_decoder
	rm -rf ./Output
	mkdir ./Output
	./jpeg_decoder ./Image/gig-sn01.jpg ./Output/gig-sn01.bmp
	./jpeg_decoder ./Image/gig-sn08.jpg ./Output/gig-sn08.bmp
	./jpeg_decoder ./Image/monalisa.jpg ./Output/monalisa.bmp
	./jpeg_decoder ./Image/teatime.jpg  ./Output/teatime.bmp

clean:
	rm jpeg_decoder
