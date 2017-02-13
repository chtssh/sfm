static const float wratios[] = {0.25, 0.35};

static struct key keys[] = {
	/* keys		function	argument */
	{ "h",		move_h,		{.i = -1} },
	{ "j",		move_v,		{.i = +1} },
	{ "k",		move_v,		{.i = -1} },
	{ "l",		move_h,		{.i = +1} },
	{ "q",		quit,		{0} },
};
