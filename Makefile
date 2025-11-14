main: self_healing_kernel.c
	@gcc self_healing_kernel.c -o shk

run: shk
	@./shk
	
clean: 
	@rm -f shk
