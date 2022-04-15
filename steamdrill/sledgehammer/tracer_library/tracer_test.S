// We need there to be a global _tracerState !!

	.data
stack:
	.long 0 /* variable declared to point to my stack*/
tempEsp:
	.long 0 /* for temporarily holding esp */
	.global stack
	.global tempEsp
	.text
	.global tracer_push_registers
tracer_push_registers:
	mov %esp, (tempEsp)
	mov stack, %esp
	mov (%esp), %esp
	jmp stack
//	push %eax
//	push %ecx
//	push %edx
//	push %ebx
//	push %ebp
//	push %esi
//	push %edi
	pusha
	ret
	.type tracer_push_registers, STT_FUNC

	.global tracer_pop_registers
tracer_pop_registers:
//	pop %edi
//	pop %esi
//	pop %ebp
//	pop %ebx
//	pop %edx
//	pop %ecx
//	pop %eax
//	pop %esp
	popa
	mov tempEsp, %esp
	ret
	.type tracer_pop_registers, STT_FUNC
