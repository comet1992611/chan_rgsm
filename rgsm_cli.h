#ifndef RGSM_CLI_H_INCLUDED
#define RGSM_CLI_H_INCLUDED

/**
 * RGSM Asterisk CLI
*/

char* cli_show_modinfo(struct ast_cli_entry* e, int cmd, struct ast_cli_args* a);
char* cli_show_channels(struct ast_cli_entry* e, int cmd, struct ast_cli_args* a);
char* cli_show_channel(struct ast_cli_entry* e, int cmd, struct ast_cli_args* a);
char* cli_channel_actions(struct ast_cli_entry* e, int cmd, struct ast_cli_args* a);
char* cli_device_actions(struct ast_cli_entry* e, int cmd, struct ast_cli_args* a);

//May 24, 2013: Bz1934 - Usefull table outputs
char* cli_show_netinfo(struct ast_cli_entry* e, int cmd, struct ast_cli_args* a);
char* cli_show_calls(struct ast_cli_entry* e, int cmd, struct ast_cli_args* a);
char* cli_show_devinfo(struct ast_cli_entry* e, int cmd, struct ast_cli_args* a);


static struct ast_cli_entry chan_rgsm_cli[] = {
	AST_CLI_DEFINE(cli_show_modinfo, "Show RGSM module information"),
    AST_CLI_DEFINE(cli_show_channels, "Show RGSM channels"),
	AST_CLI_DEFINE(cli_show_channel, "Show RGSM channel"),
    AST_CLI_DEFINE(cli_show_netinfo, "Show RGSM channels gsm network info"),
	AST_CLI_DEFINE(cli_show_calls, "Show RGSM channels call"),
	AST_CLI_DEFINE(cli_show_devinfo, "Show RGSM channels device info"),
	AST_CLI_DEFINE(cli_channel_actions, "RGSM channel actions"),
	AST_CLI_DEFINE(cli_device_actions, "RGSM GGW8 device actions"),
};

#endif // RGSM_CLI_H_INCLUDED
