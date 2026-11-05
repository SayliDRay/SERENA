PYTHON ?= python
ENV ?= xiao_jury_demo
PROJECT_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))
MANAGE = $(PYTHON) "$(PROJECT_ROOT)manage.py"

.PHONY: help setup setup-dev run test build check scan doctor clean
help:
	@echo "SERENA bracelet - available commands"
	@echo "  make run        Start the 3D bracelet and calling service"
	@echo "  make setup      Install runtime dependencies"
	@echo "  make setup-dev  Install firmware build dependencies"
	@echo "  make test       Run software tests without phone calls"
	@echo "  make build      Build the jury ESP32 firmware"
	@echo "  make check      Scan sources and run tests"
	@echo "  make scan       Check publishable files for credentials"
	@echo "  make doctor     Show available local tools"
	@echo "  make clean      Remove generated caches"
	@echo "Build profile: make build ENV=xiao_esp32c3"
	@echo "Without Make: python manage.py --help"
setup:
	$(MANAGE) setup
setup-dev:
	$(MANAGE) setup-dev
run:
	$(MANAGE) run
test:
	$(MANAGE) test
build:
	$(MANAGE) build --environment $(ENV)
check:
	$(MANAGE) check
scan:
	$(MANAGE) scan
doctor:
	$(MANAGE) doctor
clean:
	$(MANAGE) clean
