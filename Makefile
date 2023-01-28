term: term.c cpu.c 
	gcc term.c $(CFLAGS) $(IFLAGS) $(LDFLAGS) -lpthread -o term
vm: vm.c cpu.c
	gcc vm.c $(CFLAGS) $(IFLAGS) $(LDFLAGS) -o vm
