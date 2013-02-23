SECTIONS {
  __machines_begin = ADDR(machines);
  __machines_end = __machines_begin + SIZEOF(machines);
  __cpus_begin = ADDR(cpus);
  __cpus_end = __cpus_begin + SIZEOF(cpus);
  __controllers_begin = ADDR(controllers);
  __controllers_end = __controllers_begin + SIZEOF(controllers);
}
INSERT AFTER .text;

