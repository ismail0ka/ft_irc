# **************************************************************************** #
#                                                                              #
#    Makefile                                                     ft_irc       #
#                                                                              #
#    make          build ./ircserv                                             #
#    make test     build and run the parser unit tests                         #
#    make e2e      build, then drive a real server through tests/e2e.py        #
#                                                                              #
# **************************************************************************** #

NAME		:= ircserv
TESTNAME	:= test_parser

CXX			:= c++
CXXFLAGS	:= -Wall -Wextra -Werror -std=c++98
CPPFLAGS	:= -Iinclude

SRCDIR		:= src
OBJDIR		:= build

SRCS		:= \
	$(SRCDIR)/main.cpp \
	$(SRCDIR)/irc_utils.cpp \
	$(SRCDIR)/Message.cpp \
	$(SRCDIR)/MessageParser.cpp \
	$(SRCDIR)/CommandDispatcher.cpp \
	$(SRCDIR)/Client.cpp \
	$(SRCDIR)/Channel.cpp \
	$(SRCDIR)/Server.cpp \
	$(SRCDIR)/commands/registration.cpp \
	$(SRCDIR)/commands/messaging.cpp \
	$(SRCDIR)/commands/channels.cpp \
	$(SRCDIR)/commands/oper.cpp

OBJS		:= $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
DEPS		:= $(OBJS:.o=.d)

TEST_SRCS	:= \
	tests/test_parser.cpp \
	$(SRCDIR)/Message.cpp \
	$(SRCDIR)/MessageParser.cpp

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

# -MMD -MP records the header dependencies, so editing a .hpp rebuilds every
# .cpp that includes it instead of silently linking a stale object file.
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

$(TESTNAME): $(TEST_SRCS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $(TEST_SRCS)

test: $(TESTNAME)
	./$(TESTNAME)

e2e: $(NAME)
	python3 tests/e2e.py ./$(NAME)

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME) $(TESTNAME)

re: fclean all

-include $(DEPS)

.PHONY: all test e2e clean fclean re
