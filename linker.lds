SECTIONS {
  __machines_begin = ADDR(machines);
  __machines_end = __machines_begin + SIZEOF(machines);
}
INSERT AFTER .text;

