main: main.cc user-odb.cxx
	c++ $^ -o $@ -I../../../odb/ -L/lib -l:libodb-mysql-2.5.so -l:libodb-2.5.so -l:libodb-boost-2.5.so -lmysqlclient -lfmt -lgflags -lspdlog -lpthread