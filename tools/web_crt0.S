.syntax unified
.cpu cortex-m7
.fpu fpv5-sp-d16
.thumb

.section .text._start, "ax", %progbits
.global _start
.type _start, %function
.align 2
_start:
  push {r4, lr}

  ldr r1, =_data_section_start_flash
  ldr r2, =_data_section_start_ram
  ldr r3, =_data_section_end_ram
.Lcopy_data:
  cmp r2, r3
  bcs .Lzero_bss_init
  ldrb r0, [r1], #1
  strb r0, [r2], #1
  b .Lcopy_data

.Lzero_bss_init:
  ldr r2, =_bss_section_start_ram
  ldr r3, =_bss_section_end_ram
  movs r1, #0
.Lzero_bss:
  cmp r2, r3
  bcs .Lcall_main
  strb r1, [r2], #1
  b .Lzero_bss

.Lcall_main:
  movs r0, #0
  movs r1, #0
  bl main
  pop {r4, pc}
.size _start, . - _start

.section .text.eadk_display_push_rect, "ax", %progbits
.global eadk_display_push_rect
.type eadk_display_push_rect, %function
.align 2
eadk_display_push_rect:
  sub sp, #8
  add r3, sp, #8
  stmdb r3, {r0, r1}
  svc #19
  add sp, #8
  bx lr
.size eadk_display_push_rect, . - eadk_display_push_rect

.section .text.eadk_display_push_rect_uniform, "ax", %progbits
.global eadk_display_push_rect_uniform
.type eadk_display_push_rect_uniform, %function
.align 2
eadk_display_push_rect_uniform:
  sub sp, #8
  add r3, sp, #8
  stmdb r3, {r0, r1}
  svc #20
  add sp, #8
  bx lr
.size eadk_display_push_rect_uniform, . - eadk_display_push_rect_uniform

.section .text.eadk_timing_millis, "ax", %progbits
.global eadk_timing_millis
.type eadk_timing_millis, %function
.align 2
eadk_timing_millis:
  movs r2, #0
  movs r3, #0
  push {r0, r1, r4, lr}
  mov r4, sp
  strd r2, r3, [sp]
  svc #34
  str r0, [r4, #0]
  str r1, [r4, #4]
  ldrd r0, r1, [r4]
  add sp, #8
  pop {r4, pc}
.size eadk_timing_millis, . - eadk_timing_millis

.section .text.eadk_keyboard_scan, "ax", %progbits
.global eadk_keyboard_scan
.type eadk_keyboard_scan, %function
.align 2
eadk_keyboard_scan:
  movs r2, #0
  movs r3, #0
  push {r0, r1, r4, lr}
  mov r4, sp
  strd r2, r3, [sp]
  svc #48
  str r0, [r4, #0]
  str r1, [r4, #4]
  ldrd r0, r1, [r4]
  add sp, #8
  pop {r4, pc}
.size eadk_keyboard_scan, . - eadk_keyboard_scan

.section .text.eadk_timing_msleep, "ax", %progbits
.global eadk_timing_msleep
.type eadk_timing_msleep, %function
.align 2
eadk_timing_msleep:
  svc #49
  bx lr
.size eadk_timing_msleep, . - eadk_timing_msleep

.section .text.eadk_display_draw_string, "ax", %progbits
.global eadk_display_draw_string
.type eadk_display_draw_string, %function
.align 2
eadk_display_draw_string:
  push {r4, r5}
  ldr r4, .Ltramp_ptr
  ldrh.w r5, [sp, #8]
  ldr r4, [r4, #0]
  str r5, [sp, #8]
  mov ip, r4
  pop {r4, r5}
  bx ip
.align 2
.Ltramp_ptr:
  .word _userland_trampoline_address
.size eadk_display_draw_string, . - eadk_display_draw_string
