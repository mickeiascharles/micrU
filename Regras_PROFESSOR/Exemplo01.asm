# Programa de exemplo

.data
mensagem:   .asciiz "Resultado: "    # mensagem exibida
valor:      .word 10

.text
main:
    li   $t0, 5       # primeiro valor
    li   $t1, 10      # segundo valor

    add  $t2, $t0, $t1



