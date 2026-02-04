CC=$(CROSS_COMPILE)gcc

SRC = $(wildcard /home/book/nfs_rootfs/apps/test/mqttclient/*.c \
        /home/book/nfs_rootfs/apps/test/mqttclient/common/*.c \
        /home/book/nfs_rootfs/apps/test/mqttclient/common/log/*.c \
        /home/book/nfs_rootfs/apps/test/mqttclient/common/log/arch/linux/*.c \
        /home/book/nfs_rootfs/apps/test/mqttclient/network/mbedtls/library/*.c \
        /home/book/nfs_rootfs/apps/test/mqttclient/network/mbedtls/wrapper/*.c \
        /home/book/nfs_rootfs/apps/test/mqttclient/mqtt/*.c \
        /home/book/nfs_rootfs/apps/test/mqttclient/mqttclient/*.c \
        /home/book/nfs_rootfs/apps/test/mqttclient/network/*.c \
        /home/book/nfs_rootfs/apps/test/mqttclient/platform/linux/*.c \
        )

INC =   -lpthread \
        -I/home/book/nfs_rootfs/apps/test/mqttclient/common \
        -I/home/book/nfs_rootfs/apps/test/mqttclient/common/log \
        -I/home/book/nfs_rootfs/apps/test/mqttclient/network/mbedtls/include \
        -I/home/book/nfs_rootfs/apps/test/mqttclient/network/mbedtls/include/mbedtls \
        -I/home/book/nfs_rootfs/apps/test/mqttclient/network/mbedtls/wrapper \
        -I/home/book/nfs_rootfs/apps/test/mqttclient/mqtt \
        -I/home/book/nfs_rootfs/apps/test/mqttclient/mqttclient \
        -I/home/book/nfs_rootfs/apps/test/mqttclient/network \
        -I/home/book/nfs_rootfs/apps/test/mqttclient/platform/linux \
        -I/home/book/nfs_rootfs/apps/test/mqttclient/test

OBJS = $(patsubst %.c, %.o, $(SRC))

FLAG = -g -fpic -I. -Iinclude $(INC) 
TARGET = libmqttclient.so


$(TARGET):$(OBJS)
	$(CC) -shared $^ -o $@

%.o:%.c
	$(CC) -c $(FLAG) $^ -o $@

clean:
	rm -rf $(TARGET) $(OBJS)

install:
	sudo cp -rdf $(TARGET) /usr/lib/.

remove:
	sudo rm -rdf /usr/lib/$(TARGET)

.PHONY:clean
