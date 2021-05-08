build:
	gcc -Wall \
	./*.c \
	-o game \
	-lm \
	-lSDL2 \
	-lSDL2_image \
	-lSDL2_ttf \
	-lSDL2_mixer;

clean:
	rm ./game

run:
	./game

