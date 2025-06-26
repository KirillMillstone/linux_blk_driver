APP_NAME = ioctl

$(APP_NAME).o: $(APP_NAME).c
	gcc -c $(APP_NAME).c -g -o $(APP_NAME).o

$(APP_NAME): $(APP_NAME).o
	gcc $(APP_NAME).o -g -o $(APP_NAME)

build: $(APP_NAME)

clean:
	rm $(APP_NAME)

run: $(APP_NAME)
	@echo "Execute $(APP_NAME)"
	sudo ./$(APP_NAME)