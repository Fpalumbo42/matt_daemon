NAME     = Matt_daemon
CXX      = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++20
SRCS     = main.cpp Tintin_reporter.cpp MattDaemon.cpp
OBJS     = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJS): MattDaemon.hpp Tintin_reporter.hpp

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
