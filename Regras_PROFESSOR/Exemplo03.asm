# Exemplo de programa

.data

msg:    .asciiz "Resultado # obtido"   # mensagem

.text
main:
        li      $t0, 5

        li      $t1, 10      # segundo número
        add     $t2, $t0, $t1
