static const unsigned column_ratios[] = { 2, 3, 3 };

static const unsigned sort = SORTBY_NAME;
static const unsigned sort_case_insensitive = 1;
static const unsigned sort_directories_first = 1;

static const unsigned show_hidden = 0;

static struct clrscheme clrschemes[CS_LAST] = {
	[CS_REGULAR]	= { TB_DEFAULT,			TB_DEFAULT },
	[CS_DIR]	= { TB_BLUE | TB_BOLD,		TB_DEFAULT },
	[CS_LNKDIR]	= { TB_CYAN | TB_BOLD,		TB_DEFAULT },
	[CS_EXE]	= { TB_GREEN | TB_BOLD,		TB_DEFAULT },
	[CS_CHR]	= { TB_YELLOW | TB_BOLD,	TB_DEFAULT },
	[CS_BLK]	= { TB_YELLOW | TB_BOLD,	TB_DEFAULT },
	[CS_FIFO]	= { TB_YELLOW,			TB_DEFAULT },
	[CS_SOCK]	= { TB_YELLOW,			TB_DEFAULT },
	[CS_LNKNE]	= { TB_MAGENTA | TB_BOLD,	TB_DEFAULT },
	[CS_DIREMPTY]	= { TB_DEFAULT,			TB_DEFAULT },
	[CS_DIRERR]	= { TB_DEFAULT,			TB_RED },
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
	{ "ob",		sortby,		{.u = SORTBY_NAME } },
	{ "os",		sortby,		{.u = SORTBY_SIZE } },
	{ "zs",		sort_caseins,	{.i = TOGGLE } },
	{ "zd",		sort_dirfirst,	{.i = TOGGLE } },
	{ "\x08",	show_hid,	{.i = TOGGLE } },
	{ "q",		quit,		{0} },
};
