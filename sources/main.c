#include <3ds.h>
#include <string.h>
#include <stdio.h>
#include "plgldr.h"
#include "csvc.h"
#include "common.h"

static PluginMenu   menu;
static Handle       thread;
static Handle       memLayoutChanged;
static u8           stack[STACK_SIZE] ALIGN(8);

s32     PLGLDR__FetchEvent(void);
void    PLGLDR__Reply(s32 event);

void     Flash(u32 color)
{
    color |= 0x01000000;
    for (u32 i = 0; i < 64; i++)
    {
        REG32(0x10202204) = color;
        svcSleepThread(5000000);
    }
    REG32(0x10202204) = 0;
}

// Add an entry in the menu
void    NewEntry(const char *name, const char *hint)
{
    if (menu.nbItems >= MAX_ITEMS_COUNT)
        return;

    u32 index = menu.nbItems;

    menu.states[index] = 0;
    if (name)
        strncpy(menu.items[index], name, MAX_BUFFER);
    if (hint)
        strncpy(menu.hints[index], hint, MAX_BUFFER);
    ++menu.nbItems;
}

// Create the menu
void    InitMenu(void)
{
    memset(&menu, 0, sizeof(menu));

    strncpy(menu.title, "Sample plugin", MAX_BUFFER);

    NewEntry("Flash green - L", "Press L to get a green flash");
    NewEntry("Flash red - Left", "Press Left to get a red flash");
    NewEntry("Flash blue - Right", "Press Right to get a blue flash");

    for (u32 i = 0; i < 25; ++i)
    {
        char buffer[50];

        sprintf(buffer, "Sample cheat #%d", i);
        NewEntry(buffer, NULL);
    }
}

// Apply enabled cheat
void    ApplyCheat(void)
{
    if (menu.states[0] && HID_PAD & BUTTON_L1) Flash(0x00FF00);
    if (menu.states[1] && HID_PAD & BUTTON_LEFT) Flash(0x0000FF);
    if (menu.states[2] && HID_PAD & BUTTON_RIGHT) Flash(0xFF0000);
}

// Plugin main thread entrypoint
void    ThreadMain(void *arg)
{
    // Init our menu
    InitMenu();

    while (true)
    {
        if (svcWaitSynchronization(memLayoutChanged, 10000000ULL) == 0x09401BFE) // 0.01s
        {
            s32 event = PLGLDR__FetchEvent();

            if (event == PLG_SLEEP_ENTRY)
            {
                // Add code to handle sleep mode here.

                PLGLDR__Reply(event);
            }
            else if (event == PLG_SLEEP_EXIT)
            {
                // Add code to handle waking from sleep here.

                PLGLDR__Reply(event);
            }
            else if (event == PLG_ABOUT_TO_SWAP)
            {
                // Add code to save state and get ready for dumping memory to SD.

                // Reply and wait for resuming
                PLGLDR__Reply(event);

                // Add code to restore state saved previously.
            }
            else if (event == PLG_ABOUT_TO_EXIT)
            {
                // Add code here to handle exiting the plugin

                // This function do not return and exit the thread
                PLGLDR__Reply(event);
            }

            // Check keys, display the menu if necessary
            if (HID_PAD & BUTTON_SELECT)
                PLGLDR__DisplayMenu(&menu);

            // Apply enabled cheat
            ApplyCheat();

            continue;
        }
    }
}

extern char* fake_heap_start;
extern char* fake_heap_end;
extern u32 __ctru_heap;
extern u32 __ctru_linear_heap;

u32 __ctru_heap_size        = 0;
u32 __ctru_linear_heap_size = 0;

void    __system_allocateHeaps(PluginHeader *header)
{
    __ctru_heap_size = header->heapSize;
    __ctru_heap = header->heapVA;

    // Set up newlib heap
    fake_heap_start = (char *)__ctru_heap;
    fake_heap_end = fake_heap_start + __ctru_heap_size;
}

// Entrypoint, game will starts when you exit this function
void    main(void)
{
    PluginHeader *header = (PluginHeader *)0x07000000;

    if (header->magic != HeaderMagic)
        return; ///< Abort plugin as something went wrong

    // Init heap
    __system_allocateHeaps(header);

    // Init services
    srvInit();
    plgLdrInit();

    // Get memory layout changed event
    svcControlProcess(CUR_PROCESS_HANDLE, PROCESSOP_GET_ON_MEMORY_CHANGE_EVENT, (u32)&memLayoutChanged, 0);

    // Create the plugin's main thread
    svcCreateThread(&thread, ThreadMain, 0, (u32 *)(stack + STACK_SIZE), 30, -1);
}
