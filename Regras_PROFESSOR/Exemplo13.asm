.data
mensagem: .asciiz "Teste"
valor: .word 10

.text
main:
li $t0, 5
inicio:
add $t1, $t0, $t0
fim: