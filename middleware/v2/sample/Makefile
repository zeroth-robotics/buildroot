
all:
	@echo "##############################"
	@echo "#                            #"
	@echo "#   Compiling 'samples'...   #"
	@echo "#                            #"
	@echo "##############################"
	@cd common; make all || exit 1; cd ../;
	@for x in `find -L ./ -maxdepth 2 -mindepth 2 -name "Makefile" `; do \
		if [ "$$x" = "./mipi_tx/Makefile" ] && [ "${CHIP_ARCH}" = "CV180X" ]; then \
			continue; \
		fi; \
		cd `dirname $$x`; \
		if [ $$? ]; then \
			$(MAKE) all || exit 1; \
			cd ../; \
		fi; \
	done

clean:
	@for x in `find -L ./ -maxdepth 2 -mindepth 2 -name "Makefile" `; do \
		cd `dirname $$x`; \
		if [ $$? ]; then \
			$(MAKE) clean; \
			cd ../; \
		fi; \
	done
