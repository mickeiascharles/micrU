# teste

.data
msg1:   .asciiz   "#"
msg2:   .asciiz   "###"
msg3:   .asciiz   "A # B"       # comentário real
msg4:   .asciiz   "A     B"

.text

main:
li    $t0,    1      # valor