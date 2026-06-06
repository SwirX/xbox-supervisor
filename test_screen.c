/* Minimal libxenon screen test — standalone ELF.
 * Build with standard devkitxenon toolchain.
 * Run from XeLL USB boot (xenon.elf). */

#include <stdio.h>
#include <xenos/xenos.h>
#include <xenos/xe.h>
#include <xenos/edram.h>
#include <console/console.h>
#include <time/time.h>

void main(void)
{
    struct XenosDevice xe;

    /* Initialize GPU display */
    xenos_init(VIDEO_MODE_AUTO);

    /* Initialize GPU 3D engine (needed for resolve) */
    Xe_Init(&xe);
    edram_init(&xe);

    /* Initialize text console on framebuffer */
    console_init();

    /* Clear screen and print messages */
    console_clrscr();
    printf("SUPERVISOR SCREEN TEST\n");
    printf("======================\n");
    printf("\n");
    printf("If you can read this, GPU init works.\n");
    printf("\n");
    printf("Core 0 is running on PIR 0.\n");
    printf("Xenos GPU initialized successfully.\n");
    printf("\n");
    printf("Next: Core 1 bootstrap + ELF loader.\n");

    /* Spin forever */
    while (1) {
        udelay(1000000);
    }
}
