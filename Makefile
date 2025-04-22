# Compiler settings
CXX = g++

# Compiler flags for maximum speed
CXXFLAGS = -m64 -std=c++17 -Ofast -march=native -mtune=native \
           -funroll-loops -ftree-vectorize -fstrict-aliasing -fno-semantic-interposition \
           -fvect-cost-model=unlimited -fno-trapping-math -fipa-ra -flto \
           -fassociative-math -ffast-math -pthread \
           -mavx2 -mbmi2 -madx -maes -mpclmul -mfma \
           -Wall -Wextra -Werror 

# Linker flags
LDFLAGS = -pthread -flto -maes -mavx2 -mbmi2 -madx -mpclmul -mfma

TARGET = WIFRoulette
CXXFLAGS += -fPIC

# Source files
SRCS =  WIFRoulette.cpp sha256_avx2.cpp 
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

# Targets
all: $(TARGET)
	@$(MAKE) clean_intermediates

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Include dependencies
-include $(DEPS)

# Clean intermediate files only
clean_intermediates:
	rm -f $(OBJS) $(DEPS) && chmod +x $(TARGET)

# Clean everything )
clean: clean_intermediates
	rm -f $(TARGET)

.PHONY: all clean clean_intermediates