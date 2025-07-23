#ifndef ERR_H
#define ERR_H

enum {
    VM_OK = 0,
	VM_INT_DIV,
    VM_INT_INVOPC,
    VM_INT_PF,
    VM_INT_PROT,
};

extern int vm_error;

#endif // ERR_H