#include "menu.h"
#include <ncursesw/ncurses.h>

int main()
{
    initialize_application();
    handle_user_name();

    int count;
    MenuItem* filtered_items = create_filtered_menu(&count);

    run_menu(filtered_items, count);

    return 0;
}
