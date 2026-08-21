.data                 # seção de dados
valor: .word 10       # valor inicial

.text                 # seção de código
main:                  # início
li $t0, 5              # primeiro valor
li $t1, 10             # segundo valor
add $t2, $t0, $t1     # realiza a soma