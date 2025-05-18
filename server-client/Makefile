default: image_client_exe image_server_exe video_client_exe video_server_exe

image_client_exe:
	cc image_client.c -o $@ \
		-I/Users/rohit/Github/thirdparty/zmq/include \
		-L/Users/rohit/Github/thirdparty/zmq/lib \
		-Wl,-rpath,/Users/rohit/Github/thirdparty/zmq/lib\
	   	-lzmq

image_server_exe:
	cc image_server.c -o $@ \
		-I/Users/rohit/Github/thirdparty/zmq/include \
		-L/Users/rohit/Github/thirdparty/zmq/lib \
		-Wl,-rpath,/Users/rohit/Github/thirdparty/zmq/lib\
	   	-lzmq

video_client_exe:
	cc video_client.c -o $@ \
		-I/Users/rohit/Github/thirdparty/zmq/include \
		-I/opt/homebrew/include \
		-L/opt/homebrew/lib \
		-L/Users/rohit/Github/thirdparty/zmq/lib \
		-Wl,-rpath,/Users/rohit/Github/thirdparty/zmq/lib \
		-framework OpenGL\
		-D GL_SILENCE_DEPRECATION\
		-lglfw -lGL

video_server_exe:
	cc video_server.c -o $@ \
		-I/Users/rohit/Github/thirdparty/zmq/include \
		-I/opt/homebrew/include \
		-L/opt/homebrew/lib \
		-L/Users/rohit/Github/thirdparty/zmq/lib \
		-Wl,-rpath,/Users/rohit/Github/thirdparty/zmq/lib \
		-framework OpenGL\
		-D GL_SILENCE_DEPRECATION\
		-lavcodec -lavformat -lavutil -lswscale

clean:
	rm -f image_client_exe video_client_exe video_server_exe
