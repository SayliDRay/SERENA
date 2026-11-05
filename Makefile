PYTHON ?= python
ENV ?= xiao_jury_demo

.PHONY: help setup setup-dev run test build check scan doctor clean
help:
	@echo "SERENA bracelet: make run, test, build, check, doctor, clean"
	@echo "Without make: python manage.py COMMAND"
setup:
	$(PYTHON) manage.py setup
setup-dev:
	$(PYTHON) manage.py setup-dev
run:
	$(PYTHON) manage.py run
test:
	$(PYTHON) manage.py test
build:
	$(PYTHON) manage.py build --environment $(ENV)
check:
	$(PYTHON) manage.py check
scan:
	$(PYTHON) manage.py scan
doctor:
	$(PYTHON) manage.py doctor
clean:
	$(PYTHON) manage.py clean
