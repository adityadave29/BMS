# Top-level Makefile for Bank Management System (BMS)

.PHONY: all server client run-server run-client clean help

all: server client

server:
	$(MAKE) -C server

client:
	$(MAKE) -C client

run-server: server
	$(MAKE) -C server run

run-client: client
	$(MAKE) -C client run

clean:
	$(MAKE) -C server clean
	$(MAKE) -C client clean

help:
	@echo "Bank Management System Build Targets:"
	@echo "  make            - Build both server and client"
	@echo "  make server     - Build server executable"
	@echo "  make client     - Build client executable"
	@echo "  make run-server - Build and run server"
	@echo "  make run-client - Build and run client"
	@echo "  make clean      - Remove build artifacts"
