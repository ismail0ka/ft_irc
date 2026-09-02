NAME		:= ircserv
TEST_NAME	:= test_parser
UNIT_NAME	:= test_client

CXX			:= c++
CXXFLAGS	:= -Wall -Wextra -Werror -std=c++98


CPPFLAGS	:= -Iincludes -MMD -MP

SRC_DIR		:= srcs
INC_DIR		:= includes
OBJ_DIR		:= obj


SRCS		:=	$(SRC_DIR)/main.cpp \
				$(SRC_DIR)/Server.cpp \
				$(SRC_DIR)/Client.cpp \
				$(SRC_DIR)/Message.cpp \
				$(SRC_DIR)/MessageParser.cpp \
				$(SRC_DIR)/CommandDispatcher.cpp \
				$(SRC_DIR)/irc_utils.cpp \
				$(SRC_DIR)/registration.cpp \
				$(SRC_DIR)/messaging.cpp \
				$(SRC_DIR)/channels.cpp \
				$(SRC_DIR)/oper.cpp \
				$(SRC_DIR)/Channel.cpp

TEST_SRCS	:=	tests/test_parser.cpp \
				$(SRC_DIR)/Message.cpp \
				$(SRC_DIR)/MessageParser.cpp

UNIT_SRCS	:=	tests/test_client.cpp \
				$(SRC_DIR)/Client.cpp

OBJS		:= $(SRCS:%.cpp=$(OBJ_DIR)/%.o)
TEST_OBJS	:= $(TEST_SRCS:%.cpp=$(OBJ_DIR)/%.o)
UNIT_OBJS	:= $(UNIT_SRCS:%.cpp=$(OBJ_DIR)/%.o)
DEPS		:= $(OBJS:.o=.d) $(TEST_OBJS:.o=.d) $(UNIT_OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

$(TEST_NAME): $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_OBJS)

$(UNIT_NAME): $(UNIT_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(UNIT_OBJS)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# unit tests: no sockets, no server
test: $(TEST_NAME) $(UNIT_NAME)
	@./$(TEST_NAME)
	@./$(UNIT_NAME)

# integration tests: drives a real ./ircserv over real sockets
nettest: $(NAME)
	@./tests/net_test.sh

testall: test nettest

debug:
	$(MAKE) fclean
	$(MAKE) all CXXFLAGS="$(CXXFLAGS) -g3 -fsanitize=address,undefined"

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME) $(TEST_NAME) $(UNIT_NAME)

re:
	$(MAKE) fclean
	$(MAKE) all

-include $(DEPS)

.PHONY: all test nettest testall debug clean fclean re
