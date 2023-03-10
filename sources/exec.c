/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   exec.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: zrebhi <zrebhi@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/01/04 13:17:49 by zrebhi            #+#    #+#             */
/*   Updated: 2023/03/10 17:45:52 by zrebhi           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../includes/minishell.h"

int		ft_builtins(t_minishell *data);

static int	is_a_directory(char *path)
{
	struct stat	path_stat;

	path_stat.st_mode = 0;
	stat(path, &path_stat);
	if (S_ISDIR(path_stat.st_mode))
	{
		ft_putstr_fd("msh: ", 2);
		ft_putstr_fd(path, 2);
		ft_putstr_fd(" is a directory\n", 2);
		return (1);
	}
	return (0);
}

/* Handles the executions of the commands and returns the corresponding error
if the command was not found. */

void	ft_exec(t_minishell *data)
{
	int		i;
	char	*cmd;
	
	if (!ft_strncmp(data->cmds->full_cmd[0], "./", 2))
	{
		if (is_a_directory(data->cmds->full_cmd[0] + 2))
			return (exit(126));
		if (access(data->cmds->full_cmd[0], X_OK) == -1)
			return (perror(data->cmds->full_cmd[0]), exit(126));
		execve(data->cmds->full_cmd[0], data->cmds->full_cmd, data->envp);
		exit(0);
	}
	execve(data->cmds->full_cmd[0], data->cmds->full_cmd, data->envp);
	data->paths = ft_pathfinder(&data->head_env);
	i = -1;
	if (data->paths)
	{
		while (data->paths[++i])
		{
			cmd = ft_strjoin(data->paths[i], data->cmds->full_cmd[0]);
			if (!cmd)
				return ;
			execve(cmd, data->cmds->full_cmd, data->envp);
			free(cmd);
		}
	}
	ft_putstr_fd("command not found: ", 2);
	if (data->cmds->full_cmd[0])
		ft_putstr_fd(data->cmds->full_cmd[0], 2);
	ft_putstr_fd("\n", 2);
	exit(127);
}

/* Handles everything related to children processes. */

void	ft_incubator(t_minishell *data)
{
	if (close(data->end[0]) == -1)
		return (perror("close pipe"));
	if (data->cmds->infile > 0)
		if (dup2(data->cmds->infile, STDIN_FILENO) == -1)
			return ;
	if (data->cmds->outfile > 1)
		if (dup2(data->cmds->outfile, STDOUT_FILENO) == -1)
			return ;
	if (data->cmds->outfile == 0)
		if (dup2(data->end[1], STDOUT_FILENO) == -1)
			return (perror("dup2 end[1]"));
	if (close(data->end[1]) == -1)
		return ((void)perror("close pipe"));
	if (ft_builtins(data))
		exit(0);
	ft_exec(data);
}

int	pipex_heredoc(t_minishell *data)
{
	if (data->cmds->here_doc >= 1)
	{
		if (!data->cmds->limiter || data->cmds->error)
			return (0);
		if (dup2(data->cmds->here_doc_pipe[0], STDIN_FILENO) == -1)
			return (perror("dup2 heredoc here"), 0);
		if (close(data->cmds->here_doc_pipe[0]) == -1)
			return (perror("close pipe"), 0);
	}
	return (1);
}

/* Creates a child process for every command and links them together
with pipes */

void	set_exec_signals(void);

void	pipex_commands(t_minishell *data)
{
	while (data->cmds)
	{
		if (data->cmds->error)
			data->cmds = data->cmds->next;
		if (!pipex_heredoc(data))
			return ;
		if (pipe(data->end) == -1)
			return ((void)perror("pipe"));
		data->cmds->cmd_pid = fork();
		if (data->cmds->cmd_pid == -1)
			return ((void)perror("Fork"));
		if (data->cmds->cmd_pid && close(data->end[1]) == -1)
			return ((void)perror("close pipe"));
		if (data->cmds->cmd_pid == 0)
			ft_incubator(data);
		if (!ft_strcmp(data->cmds->full_cmd[0], "./minishell"))
			unplug_signals();
		if (dup2(data->end[0], STDIN_FILENO) == -1)
			return ((void)perror("dup2 end[0]"));
		if (close(data->end[0]) == -1)
			return ((void)perror("close pipe"));
		data->cmds = data->cmds->next;
	}
}

/* Runs our commands with pipex_commands, then closes our remaining fds
and waits for children processes to be done */

void	pipex(t_minishell *data)
{
	t_cmdlist	*head;

	head = data->cmds;
	set_exec_signals();
	pipex_commands(data);
	data->cmds = head;
	while (data->cmds)
	{
		if (data->cmds->outfile > 1 && close(data->cmds->outfile) == -1)
			perror("close outfile");
		data->cmds = data->cmds->next;
	}
	data->cmds = head;
	while (data->cmds)
	{
		waitpid(data->cmds->cmd_pid, &data->status, 0);
		g_status = WEXITSTATUS(data->status);
		data->cmds = data->cmds->next;
	}
	exit (g_status);
}
