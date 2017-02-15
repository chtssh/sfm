static const float wratios[] = {0.25, 0.35};

static struct key keys[] = {
	/* keys		function	argument */
	{ "h",		move_h,		{.i = -1} },
	{ "j",		move_v,		{.i = +1} },
	{ "k",		move_v,		{.i = -1} },
	{ "l",		move_h,		{.i = +1} },
	{ "gg",		goto_line,	{.i = 0} },
	{ "G",		goto_line,	{.i = ~0} },
	{ "g/",		goto_dir,	{.v = "/"} },
	{ "gd",		goto_dir,	{.v = "/dev"} },
	{ "ge",		goto_dir,	{.v = "/etc"} },
	{ "gm",		goto_dir,	{.v = "/media"} },
	{ "gM",		goto_dir,	{.v = "/mnt"} },
	{ "go",		goto_dir,	{.v = "/opt"} },
	{ "gs",		goto_dir,	{.v = "/srv"} },
	{ "gu",		goto_dir,	{.v = "/usr"} },
	{ "gv",		goto_dir,	{.v = "/var"} },
	{ "ob",		sort_files,	{.v = cmpfi_name} },
	{ "os",		sort_files,	{.v = cmpfi_size} },
	{ "q",		quit,		{0} },
};
