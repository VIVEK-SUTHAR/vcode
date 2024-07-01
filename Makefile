vcode: vcode.c
					$(CC) vcode.c -o vcode -Wall -Wextra -pedantic -std=c99

json: file.c
					$(CC) file.c -o file -Wall -Wextra -pedantic -std=c99
					./file
