/* Common widget functions. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/inphist.h"
#include "bfu/widget.h"
#include "intl/gettext/libintl.h"
#include "terminal/terminal.h"
#include "util/error.h"


void
display_widget(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	if (widget_data->widget->ops->display)
		widget_data->widget->ops->display(dlg_data, widget_data);
}
