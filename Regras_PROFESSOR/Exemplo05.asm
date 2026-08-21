# o pré-processador não deve validar a instrução

.text

main:

    add      $t0,      $t1       # falta operando
    add      $t0,      $t1,      $t99     # registrador inválido