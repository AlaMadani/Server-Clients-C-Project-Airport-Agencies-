#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAX_BUFFER 256
#define SERVER_IP "x.x.x.x" // Replace with server IP
#define AGENCE_ID "AG2"

void send_request(int port, char *request, char *response)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
    {
        perror("Socket creation failed");
        return;
    }
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Connection failed");
        close(sockfd);
        return;
    }
    int n = write(sockfd, request, strlen(request));
    if (n < 0)
    {
        perror("Write failed");
        close(sockfd);
        return;
    }
    bzero(response, MAX_BUFFER);
    n = read(sockfd, response, MAX_BUFFER - 1);
    if (n < 0)
    {
        perror("Read failed");
        close(sockfd);
        return;
    }
    close(sockfd);
}

int main()
{
    char buffer[MAX_BUFFER], response[MAX_BUFFER];
    int choice;
    while (1)
    {
        printf("\n=== Menu Agence %s ===\n", AGENCE_ID);
        printf("1. Envoyer demande de reservation\n");
        printf("2. Annuler reservation\n");
        printf("3. Consulter les vols\n");
        printf("4. Consulter facture totale\n");
        printf("5. Consulter l'historique\n");
        printf("6. Quitter\n");
        printf("Choix : ");
        scanf("%d", &choice);
        getchar(); // Clear newline

        bzero(buffer, MAX_BUFFER);
        bzero(response, MAX_BUFFER);

        switch (choice)
        {
        case 1:
        {
            char ref_vol[10];
            int nb_places;
            printf("Numero du vol : ");
            scanf("%s", ref_vol);
            printf("Nombre de places : ");
            scanf("%d", &nb_places);
            snprintf(buffer, MAX_BUFFER, "RESERVATION %s %s %d", AGENCE_ID, ref_vol, nb_places);
            send_request(5000, buffer, response);
            printf("%s\n", response);
            break;
        }
        case 2:
        {
            char ref_vol[10];
            int nb_places;
            printf("Numero du vol : ");
            scanf("%s", ref_vol);
            printf("Nombre de places a annuler : ");

            scanf("%d", &nb_places);
            snprintf(buffer, MAX_BUFFER, "ANNULATION %s %s %d", AGENCE_ID, ref_vol, nb_places);
            send_request(5001, buffer, response);
            printf("%s\n", response);
            break;
        }
        case 3:
        {
            snprintf(buffer, MAX_BUFFER, "CONSULTER_VOLS");
            send_request(5002, buffer, response);
            printf("%s\n", response);
            break;
        }
        case 4:
        {
            snprintf(buffer, MAX_BUFFER, "CONSULTER_FACTURE %s", AGENCE_ID);
            send_request(5003, buffer, response);
            printf("%s\n", response);
            break;
        }
        case 5:
        {
            snprintf(buffer, MAX_BUFFER, "CONSULTER_HISTORIQUE");
            send_request(5004, buffer, response);
            printf("%s\n", response);
            break;
        }
        case 6:
            printf("Au revoir!\n");
            exit(0);
        default:
            printf("Choix invalide\n");
        }
    }
    return 0;
}
