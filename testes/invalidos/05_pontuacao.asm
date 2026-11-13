.data
vetor: .word 10,
lista: .word 1 2
rot .word 3
msg: .asciiz
.text
    add $t0 $t1, $t2
    lw $t0, 4 $sp
    lw $t0, 4($sp
    sw $t0, ($sp)
inicio
    syscall $t0
    nop nop
    .globl
    li $v0, 10
