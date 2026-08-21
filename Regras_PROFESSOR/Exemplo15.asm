# Programa de teste integrado

.data

mensagem:      .asciiz      "Resultado # obtido"     # mensagem
valor:         .word        10

.text

main:               # ponto inicial

        li      $t0,     5       # primeiro valor
        li      $t1,     10      # segundo valor

        add     $t2,     $t0,     $t1