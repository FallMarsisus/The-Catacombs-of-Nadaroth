gcc -fopenmp -lpthread -Wall -Wextra -Wpedantic -Wshadow -Wformat=0 -Wcast-align -Wnull-dereference -g3 -O0 main.c input_manager.c render.c inventory.c entity.c item.c handler.c player.c map.c generation.c parser.c hash.c dynarray.c -o .\build\Debug\outDebug.exe
.\run.bat