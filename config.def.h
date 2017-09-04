static const unsigned column_ratios[] = { 2, 3, 3 };

static const unsigned sort = NAV_SORTBY_NAME;
static const unsigned sort_case_insensitive = 1;
static const unsigned sort_directories_first = 1;

static const unsigned show_hidden = 0;

static struct colorscheme scheme = {
	.reg	= { COLOR_WHITE },
	.dir	= { COLOR_BLUE | ATTR_BOLD },
	.lnk	= { COLOR_CYAN },
	.exe	= { COLOR_GREEN | ATTR_BOLD },
	.chr	= { COLOR_YELLOW | ATTR_BOLD },
	.blk	= { COLOR_YELLOW | ATTR_BOLD },
	.fifo	= { COLOR_YELLOW },
	.sock	= { COLOR_YELLOW },
	.lnkne	= { COLOR_MAGENTA | ATTR_BOLD },
	.empty	= { COLOR_WHITE },
	.err	= { COLOR_WHITE, COLOR_RED }
};

static struct key keys[] = {
	/* keys		function	argument */
	{ "h",		move_h,		{.i = -1 } },
	{ "j",		move_v,		{.i = +1 } },
	{ "k",		move_v,		{.i = -1 } },
	{ "l",		move_h,		{.i = +1 } },
	{ "\x15",	move_page,	{.f = -0.5 } },
	{ "\x04",	move_page,	{.f = +0.5 } },
	{ "\x02",	move_page,	{.f = -1 } },
	{ "\x06",	move_page,	{.f = +1 } },
	{ "gg",		goto_line,	{.u = 0 } },
	{ "G",		goto_line,	{.u = ~0 } },
	{ "g/",		goto_dir,	{.v = "/" } },
	{ "gd",		goto_dir,	{.v = "/dev" } },
	{ "ge",		goto_dir,	{.v = "/etc" } },
	{ "gm",		goto_dir,	{.v = "/media" } },
	{ "gM",		goto_dir,	{.v = "/mnt" } },
	{ "go",		goto_dir,	{.v = "/opt" } },
	{ "gs",		goto_dir,	{.v = "/srv" } },
	{ "gu",		goto_dir,	{.v = "/usr" } },
	{ "gv",		goto_dir,	{.v = "/var" } },
	{ "ob",		sortby,		{.u = NAV_SORTBY_NAME } },
	{ "os",		sortby,		{.u = NAV_SORTBY_SIZE } },
	{ "zs",		sort_caseins,	{.i = TOGGLE } },
	{ "zd",		sort_dirfirst,	{.i = TOGGLE } },
	{ "\x08",	show_hid,	{.i = TOGGLE } },
	{ "q",		quit,		{0} },
};
